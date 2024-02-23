#include "allocation.h"
#include "inode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* The number of bytes in a block.
 * Do not change.
 */
#define BLOCKSIZE 4096

/* The lowest unused node ID.
 * Do not change.
 */
static int num_inode_ids = 0;

/* This helper function computes the number of blocks that you must allocate
 * on the simulated disk for a give file system in bytes. You don't have to use
 * it.
 * Do not change.
 */
static int blocks_needed(int bytes)
{
    int blocks = bytes / BLOCKSIZE;
    if (bytes % BLOCKSIZE != 0)
        blocks += 1;
    return blocks;
}

/* This helper function returns a new integer value when you create a new inode.
 * This helps you do avoid inode reuse before 2^32 inodes have been created. It
 * keeps the lowest unused inode ID in the global variable num_inode_ids.
 * Make sure to update num_inode_ids when you have loaded a simulated disk.
 * Do not change.
 */
static int next_inode_id()
{
    int retval = num_inode_ids;
    num_inode_ids += 1;
    return retval;
}

struct inode *create_file(struct inode *parent, char *name, int size_in_bytes)
{
    if (parent->filesize != 0)
    {
        perror("Parent er ikke en directory");
        exit(EXIT_FAILURE);
    }
    if (find_inode_by_name(parent, name) != NULL)
    {
        perror("Det eksisterende allerede");

        return NULL;
    }

    struct inode *nytt_file = (struct inode *)malloc(sizeof(struct inode));

    nytt_file->name = (char *)malloc(sizeof(name) + 1);
    strcpy(nytt_file->name, name);
    nytt_file->id = next_inode_id();

    nytt_file->is_directory = 0;
    nytt_file->filesize = size_in_bytes;

    nytt_file->children = NULL;
    nytt_file->num_children = 0;

    nytt_file->num_blocks = blocks_needed(size_in_bytes);

    nytt_file->blocks = (size_t *)malloc(nytt_file->num_blocks * sizeof(size_t));
    if (!nytt_file->blocks)
    {
        perror("Failed to allocate memory for file blocks");
        free(nytt_file->name);
        free(nytt_file);
        return NULL;
    }
    for (int i = 0; i < nytt_file->num_blocks; i++)
    {
        nytt_file->blocks[i] = allocate_block();
    }
    parent->children = (struct inode **)realloc(parent->children, (parent->num_children + 1) * sizeof(struct inode *));
    if (!parent->children)
    {
        perror("Failed to allocate memory for expandig parent's childeren array");
        free(nytt_file->blocks);
        free(nytt_file->name);
        free(nytt_file);
        exit(EXIT_FAILURE);
    }

    parent->children[parent->num_children] = nytt_file;
    parent->num_children++;

    return nytt_file;
}

struct inode *create_dir(struct inode *parent, char *name)
{
    if (parent->is_directory != 1)
    {
        perror("Parent er ikke en directory");
        exit(EXIT_FAILURE);
    }

    if (find_inode_by_name(parent, name) != NULL)
    {
        perror("Det allerede eksisterer!");
        exit(EXIT_FAILURE);
    }
    struct inode *ny_dir = malloc(sizeof(struct inode));
    if (ny_dir == NULL)
    {
        perror("Minne Allokasjon Feil!!");
        exit(EXIT_FAILURE);
    }
    ny_dir->name = (char *)malloc(strlen(name) + 1);
    if (ny_dir->name == NULL)
    {
        perror("Minne Allokasjon error");
        free(ny_dir);
        return NULL;
    }
    strcpy(ny_dir->name, name);
    ny_dir->id = next_inode_id();
    ny_dir->is_directory = 1;
    ny_dir->num_children = 0;
    ny_dir->children = NULL;
    ny_dir->filesize = 0;

    ny_dir->num_blocks = 0;
    ny_dir->blocks = NULL;

    if (parent != NULL)
    {
        parent->children = realloc(parent->children, (parent->num_children + 1) * sizeof(struct inode *));
        if (parent->children == NULL)
        {
            perror("Minne Allokasjon error");
            free(ny_dir->name);
            free(ny_dir);
            return NULL;
        }
        parent->children[parent->num_children] = ny_dir;
        parent->num_children++;
    }

    return ny_dir;
}

struct inode *find_inode_by_name(struct inode *parent, char *name)
{
    if (parent == NULL)
    {
        perror("Parent doesn't have any children");
        return NULL;
    }
    for (int i = 0; i < parent->num_children; i++)
    {
        struct inode *child = (struct inode *)parent->children[i];
        if (!strcmp(child->name, name))
        {
            return child;
        }
    }
    return NULL;
}

static int verified_delete_in_parent(struct inode *parent, struct inode *node)
{
    if (parent == NULL || node == NULL)
    {
        perror("Enten parent eller Node er NULL");
        return -1;
    }
    if (parent->is_directory != 1)
    {
        perror("Parent er ikke en directory");
        return -1;
    }
    int funnet = 0;
    for (int i = 0; i < parent->num_children; i++)
    {
        if (parent->children[i] == node)
        {
            funnet = 1;

            for (int j = i; j < parent->num_children - 1; j++)
            {
                parent->children[j] = parent->children[j + 1];
            }
            struct inode **frl = realloc(parent->children, (parent->num_children - 1) * sizeof(struct inode **));
            if (frl || parent->num_children == 1)
            {
                parent->children = frl;
                parent->num_children--;
            }
            else
            {
                perror("Failed to shrink parent's children array");
                return -1;
            }
            break;
        }
    }
    if (!funnet)
    {
        perror("Node funnet ikke i parent's children");
        return -1;
    }

    return 0;
}

int is_node_in_parent(struct inode *parent, struct inode *node)
{
    if (parent == NULL || node == NULL)
    {
        perror("Enten parent eller node er NULL");
        return -1;
    }
    if (parent->is_directory != 1)
    {
        perror("Parent er ikke en directory");
        return -1;
    }
    for (int i = 0; i < parent->num_children; i++)
    {
        if (parent->children[i] == node)
        {
            return 1;
        }
    }
    return 0;
}

int delete_file(struct inode *parent, struct inode *node)
{
    if (parent == NULL || node == NULL)
    {
        perror("Parent is not a directory!");
        return -1;
    }

    if (parent->is_directory != 1)
    {
        perror("Parent is not a directory!");
        return -1;
    }
    if (node->is_directory == 1)
    {
        perror("Node is a directory, not a file!");
        return -1;
    }
    if (!is_node_in_parent(parent, node))
    {
        perror("Node is not a direct child of parent!");
        return -1;
    }
    for (int i = 0; i < node->num_blocks; i++)
    {
        int status = free_block(node->blocks[i]);
        if (status != 0)
        {
            perror("Failed to free a block of the file");
        }
    }
    if (verified_delete_in_parent(parent, node) == -1)
    {
        perror("Failed to delete node from parent's children!");
        return -1;
    }

    // Free memory allocated for node
    if (node->name)
        free(node->name);
    if (node->blocks)
        free(node->blocks);
    free(node);

    return 0;
}

int delete_dir(struct inode *parent, struct inode *node)
{
    if (parent == NULL || node == NULL)
    {
        perror("Prent eller node er null!");
    }
    if (node->is_directory != 1)
    {
        perror("node er ikke en directoray!");
        return -1;
    }
    struct inode *child;
    int status = -1;
    for (int i = 0; i < node->num_children; i++)
    {
        struct inode *child = node->children[i];
    }
    if (child->is_directory)
    {
        status = delete_dir(node, child);
    }
    else
    {
        status = delete_file(node, child);
    }
    if (status != 0)
    {
        perror("feilet å slette child node!");
        return -1;
    }
    if (node->name)
    {
        free(node->name);
    }
    free(node);
    return 0;

    return 0;
}
void save_inodes_helper(FILE *file, struct inode *node)
{
    if (!node)
        return;

    fwrite(&node->id, sizeof(int), 1, file);
    fwrite(&node->is_directory, sizeof(int), 1, file);
    fwrite(node->name, strlen(node->name) + 1, 1, file);

    if (node->is_directory)
    {
        fwrite(&node->num_children, sizeof(int), 1, file);
        for (int i = 0; i < node->num_children; ++i)
        {
            save_inodes_helper(file, node->children[i]);
        }
    }
    else
    {
        fwrite(&node->filesize, sizeof(int), 1, file);
        fwrite(&node->num_blocks, sizeof(int), 1, file);
        fwrite(node->blocks, sizeof(size_t), node->num_blocks, file);
    }
}
void save_inodes(char *master_file_table, struct inode *root)
{
    FILE *file = fopen(master_file_table, "wb");
    if (!file)
    {
        perror("feilet å åpne master fil for skrive!");
        return;
    }

    save_inodes_helper(file, root);

    fclose(file);
}

struct inode *load_inodes(char *master_file_table)
{
    FILE *file = fopen(master_file_table, "rb");
    if (!file)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    struct inode **myInodes = NULL;
    struct inode **myInodes_new = NULL;
    struct inode *inode;
    int arraySize = 0;

    while (1)
    {
        inode = (struct inode *)malloc(sizeof(struct inode));

        if (!(fread((char *)&inode->id, sizeof(char), 4, file)))
        {
            free(inode);
            break;
        }

        int lengde;
        fread((char *)&lengde, sizeof(int), 1, file);
        inode->name = (char *)malloc(lengde + 1);
        fread(inode->name, sizeof(char), lengde, file);
        inode->name[lengde] = '\0';
        fread(&inode->is_directory, sizeof(char), 1, file);
        fread((char *)&inode->filesize, sizeof(int), 1, file);
        fread((char *)&inode->num_children, sizeof(int), 1, file);

        if (inode->is_directory)
        {
            inode->children = (struct inode **)malloc(inode->num_children * sizeof(struct inode *));
            for (int i = 0; i < inode->num_children; ++i)
            {
                int childId;
                fread((char *)&childId, sizeof(int), 1, file);
                inode->children[i] = myInodes[childId];
            }
        }
        else
        {
            fread((char *)&inode->num_blocks, sizeof(int), 1, file);
            inode->blocks = (size_t *)malloc(inode->num_blocks * sizeof(size_t));
            fread(inode->blocks, sizeof(size_t), inode->num_blocks, file);
        }

        arraySize += sizeof(struct inode *);
        myInodes_new = (struct inode **)realloc(myInodes, arraySize);
        if (!myInodes_new)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        myInodes = myInodes_new;
        myInodes[inode->id] = inode;
    }

    struct inode *rotnode = myInodes[0];

    // Fixing pointers
    for (long unsigned int i = 0; i < arraySize / sizeof(struct inode *); i++)
    {
        if (myInodes[i]->is_directory)
        {
            for (int j = 0; j < myInodes[i]->num_children; j++)
            {
                myInodes[i]->children[j] = myInodes[myInodes[i]->children[j]->id];
            }
        }
    }

    free(myInodes);
    fclose(file);
    return rotnode;
}

/* This static variable is used to change the indentation while debug_fs
 * is walking through the tree of inodes and prints information.
 */
static int indent = 0;

/* Do not change.
 */
void debug_fs(struct inode *node)
{
    if (node == NULL)
        return;
    for (int i = 0; i < indent; i++)
        printf("  ");

    if (node->is_directory)
    {
        printf("%s (id %d)\n", node->name, node->id);
        indent++;
        for (int i = 0; i < node->num_children; i++)
        {
            struct inode *child = (struct inode *)node->children[i];
            debug_fs(child);
        }
        indent--;
    }
    else
    {
        printf("%s (id %d size %db blocks ", node->name, node->id, node->filesize);
        for (int i = 0; i < node->num_blocks; i++)
        {
            printf("%d ", (int)node->blocks[i]);
        }
        printf(")\n");
    }
}

/* Do not change.
 */
void fs_shutdown(struct inode *inode)
{
    if (!inode)
        return;

    if (inode->is_directory)
    {
        for (int i = 0; i < inode->num_children; i++)
        {
            fs_shutdown(inode->children[i]);
        }
    }

    if (inode->name)
        free(inode->name);
    if (inode->children)
        free(inode->children);
    if (inode->blocks)
        free(inode->blocks);
    free(inode);
}
